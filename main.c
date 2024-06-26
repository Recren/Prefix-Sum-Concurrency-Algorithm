#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

int inputArraySize = 0;

void errormsg(char *msg)
{
    perror(msg);
    exit(1);
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

void arriveAndWait(int *shared_array, int *updated_array, int *barrier_array, int numCores, int idCore)
{

    // set spot in barrier to 1
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
                for (i; i < inputArraySize; i++)
                {
                    printf("%d\n", shared_array[i]);
                }
                printf("\n\n");
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
            // If any value = 2, it means we are finished
            else if (barrier_array[currentIteration] == 2)
            {
                break; // exit function
            }
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
            // We will continue to loop until we get our barrier index set back to zero (it means core zero was in the process of resetting the array meaning we are good to go)
            if (barrier_array[idCore] == 0)
            {
                break;
            }
        }
    }
}

void compute(int idCore, int *shared_array, int numCores, int *updated_array, int *barrier_array)
{
    int i;
    // i*=2 to determine the space between neighbor
    for (i = 1; i < inputArraySize; i *= 2)
    { // i will be 1, 2, 4, 8, 16, ...
        // inputArraySize - i will be the amount of computations needed to be done for the current iteration
        // if numCores > (inputArraySize - i), then idCore >= (inputArraySize - i), then that core will drop out since the cores before it will only be used

        // Check if we have less cores than computations

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
            // n = 9
            // m = 3
            // range = 2
            // carryover = 2
            // The core will need to do range + 1 computations
            int z;
            if (idCore < carryOver)
            {

                for (z = 0; z < (range + 1); z++)
                {
                    // Gets us to the right index       //adding idCore to ensure proper offset due to the values in front of it creating an offset of plus 1
                    updated_array[(idCore * (range)) + i + z + idCore] = shared_array[(idCore * (range)) + z + idCore] + shared_array[(idCore * (range)) + i + z + idCore];
                }
            }
            else
            {

                for (z = 0; z < (range); z++)
                {
                    // Gets us to the right index
                    updated_array[(idCore * (range)) + i + z + carryOver] = shared_array[(idCore * (range)) + z + carryOver] + shared_array[(idCore * (range)) + i + z + carryOver];
                }
            }
        }
        // Send the core to the waiting room
        arriveAndWait(shared_array, updated_array, barrier_array, numCores, idCore);

        // Wait for all other cores to finish before going again
    }
    // If we are here, that means the child process has exited and completed its work
    // Set to 2 to let parent know we are finished and then exit
    barrier_array[idCore] = 2;
}

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
                printf("core %d done\n", i);
                exit(0);
            }
        }
    }

    // When all children get done being made, parent will go to its barrier
    // parentBarrier(shared_array, updated_array, barrier_array, numCores);

    // When parent gets here, reap the children, then exit function
    // usleep(10000);
}

int main()
{

    // Must make sure value is greater than zero
    int numElements;
    while (1)
    {
        printf("Enter number of elements in the input array: ");
        scanf("%d", &numElements);
        if (numElements > 0)
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
    FILE *outputFile;
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

    while (1)
    {
        printf("Enter name of the output file: ");
        scanf("%s", fileName);

        // Check to see if the file opens
        outputFile = fopen(fileName, "w");
        if (outputFile != NULL)
        {
            break;
        }
        else
        {
            printf("Invalid file name\n");
        }
    }

    // Check if the input number matches the number of elements in the file and write the elements into the array
    // Our array will have to be dynamic and grow based on size of input file
    int *inputArray;
    int numElementsInFile = checkInputNumberMatches(file, numElements, &inputArray);

    // Exit program if we have invalid number of elements
    if (numElementsInFile == 0)
    {
        return 1;
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

    int *shared_array;
    int *updated_array;
    int *barrier_array;
    // create a memory segment to be shared
    int segment_idOne = shmget(IPC_PRIVATE, (inputArraySize * sizeof(int)), S_IRUSR | S_IWUSR);
    int segment_idTwo = shmget(IPC_PRIVATE, (inputArraySize * sizeof(int)), S_IRUSR | S_IWUSR);
    int segment_idThree = shmget(IPC_PRIVATE, (numCores * sizeof(int)), S_IRUSR | S_IWUSR);
    if (segment_idOne < 0 || segment_idTwo < 0 || segment_idThree < 0)
        errormsg("ERROR in creating a shared memory segment\n");

    //    fprintf(stdout, "Segment id = %d\n", segment_id);

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
    // printf("Parent PID: %d\n", pidOfRoot);

    // Spawn the processes
    spawnProcesses(numCores, pidOfRoot, shared_array, updated_array, barrier_array, childPIDS);

    // Parent process code to wait for all children to terminate
    int status;

    // If we are the parent process, wait for all the children to finish
    //  if(pidOfRoot == getpid()){
    //      for (i = 0; i < numCores; i++) {
    //          wait(NULL);
    //      }
    //      printf("All child processes have exited\n");

    // }
    usleep(100000);
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
