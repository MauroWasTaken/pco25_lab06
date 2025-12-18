#ifndef THREADEDMATRIXMULTIPLIER_H
#define THREADEDMATRIXMULTIPLIER_H
#include <list>
#include <pcosynchro/pcoconditionvariable.h>
#include <pcosynchro/pcohoaremonitor.h>
#include <pcosynchro/pcomutex.h>
#include <pcosynchro/pcosemaphore.h>
#include <pcosynchro/pcothread.h>
#include "abstractmatrixmultiplier.h"
#include "matrix.h"
///
/// A class that holds the necessary parameters for a thread to do a job.
///
template<class T>
class ComputeParameters
{
public:
    const SquareMatrix<T>* A;
    const SquareMatrix<T>* B;
    SquareMatrix<T>* C;
    /* Maybe some parameters */
    int x; // distance to left of array
    int y; // distance to top of array
    int blockSize; //size of block
};
/// As a suggestion, a buffer class that could be used to communicate between
/// the workers and the main thread...
///
/// Here we only wrote two potential methods, but there could be more at the end...
///
template<class T>
class Buffer : public PcoHoareMonitor
{
    std::list<ComputeParameters<T>> paramQueue;
    Condition notEmpty;
    Condition jobsComplete;
    Condition notBusy;
    bool isStopped = false;
    int nbJobsDispatched{0};
public:
    int nbJobFinished{0}; // Keep this updated
    /* Maybe some parameters */
    ///
    /// \brief Sends a job to the buffer
    /// \param params to a ComputeParameters object which holds the necessary parameters to execute a job
    ///
    void sendJob(ComputeParameters<T> params) {
        monitorIn();
        paramQueue.push_back(params);
        signal(notEmpty);
        monitorOut();
    }
    ///
    /// \brief Requests a job to the buffer
    /// \param parameters to a ComputeParameters object which holds the necessary parameters to execute a job
    /// \return true if a job is available, false otherwise
    ///
    bool getJob(ComputeParameters<T>& parameters) {
        monitorIn();
        if (paramQueue.empty() && !isStopped)
            wait(notEmpty);
        if (isStopped) {
            signal(notEmpty); //recursively release the other threads
            monitorOut();
            return false;
        }
        parameters = paramQueue.front();
        paramQueue.pop_front();
        nbJobsDispatched++;
        monitorOut();
        return true;
    }
    /* Maybe more methods */
    ///
    /// \brief function called when thread finishes their job
    ///
    void jobFinished() {
        monitorIn();
        nbJobsDispatched--;
        nbJobFinished++;
        if (paramQueue.empty() && nbJobsDispatched == 0) {
            signal(jobsComplete);
            signal(notBusy);
        }
        monitorOut();
    }
    ///
    /// \brief Stops all current and future jobs
    ///
    void stop() {
        monitorIn();
        isStopped = true;
        signal(notEmpty);
        signal(jobsComplete);
        signal(notBusy);
        monitorOut();
    }
    ///
    /// \brief blocking function that waits until the buffer is free to use
    ///
    void waitFree() {
        monitorIn();
        if (isStopped){
            monitorOut();
            return;
        }
        if (!paramQueue.empty() || nbJobsDispatched != 0) {
            wait(notBusy);
        }
        monitorOut();
    }
    ///
    /// \brief blocking function that waits until the jobs sent to the buffer are done running
    ///
    void waitJobs() {
        monitorIn();
        if (!paramQueue.empty() || nbJobsDispatched != 0) {
            wait(jobsComplete);
        }
        monitorOut();
    }
};
///
/// A multi-threaded multiplicator. multiply() should at least be reentrant.
/// It is up to you to offer a very good parallelism.
///
template<class T>
class ThreadedMatrixMultiplier : public AbstractMatrixMultiplier<T>
{
public:
    ///
    /// \brief ThreadedMatrixMultiplier
    /// \param nbThreads Number of threads to start
    /// \param nbBlocksPerRow Default number of blocks per row, for compatibility with SimpleMatrixMultiplier
    ///
    /// The threads shall be started from the constructor
    ///
    ThreadedMatrixMultiplier(int nbThreads, int nbBlocksPerRow = 0)
        : nbThreads(nbThreads), nbBlocksPerRow(nbBlocksPerRow), buffer()
    {
        for (int i = 0; i < nbThreads; i++) {
            threads.push_back(std::make_unique<PcoThread>(&ThreadedMatrixMultiplier::run, this));
        }
    }
    ///
    /// In this destructor we should ask for the termination of the computations. They could be aborted without
    /// ending into completion.
    /// All threads have to be
    ///
    ~ThreadedMatrixMultiplier()
    {
        buffer.stop();
        for (int i = 0; i < nbThreads; i++) {
            threads[i]->join();
        }
    }
    ///
    /// \brief multiply
    /// \param A First matrix
    /// \param B Second matrix
    /// \param C Result of AxB
    ///
    /// For compatibility reason with SimpleMatrixMultiplier
    void multiply(const SquareMatrix<T>& A, const SquareMatrix<T>& B, SquareMatrix<T>& C) override
    {
        multiply(A, B, C, nbBlocksPerRow);
    }
    ///
    /// \brief multiply
    /// \param A First matrix
    /// \param B Second matrix
    /// \param C Result of AxB
    /// \param nbBlocksPerRow Number of blocks per row (or columns)
    ///
    /// Executes the multithreaded computation, by decomposing the matrices into blocks.
    /// nbBlocksPerRow must divide the size of the matrix.
    ///
    void multiply(const SquareMatrix<T>& A, const SquareMatrix<T>& B, SquareMatrix<T>& C, int nbBlocksPerRow)
    {
        buffer.waitFree();
        int blockSize = A.getSizeX() / nbBlocksPerRow;
        for (int y = 0; y < nbBlocksPerRow; y++) {
            for (int x = 0; x < nbBlocksPerRow; x++) {
                ComputeParameters<T> params;
                params.A = &A;
                params.B = &B;
                params.C = &C;
                params.x = x * blockSize;
                params.y = y * blockSize;

                //checks makes sure last row/column is calculated even if odd
                if (x == nbBlocksPerRow - 1) {
                    params.blockSize = C.getSizeX() - params.x;
                } else if (y == nbBlocksPerRow - 1) {
                    params.blockSize = C.getSizeX() - params.y;
                } else {
                    params.blockSize = blockSize;
                }
                buffer.sendJob(params);
            }
        }
        buffer.waitJobs();
    }
protected:
    int nbThreads;
    int nbBlocksPerRow;
    std::vector<std::unique_ptr<PcoThread>> threads;
    Buffer<T> buffer;
private:
    ///
    /// \brief main logic loop for the different threads
    ///
    void run() {
        while (true) {
            ComputeParameters<T> params;
            if (!buffer.getJob(params)) {
                break;
            }
            doJob(params);
            buffer.jobFinished();
        }
    }
    ///
    /// \brief executes the job calculating the value of every cell in the block
    /// \param params object of ComputeParameters containing all information need for the job
    ///
    void doJob(ComputeParameters<T> params) {
        for (int bx = 0; bx < params.blockSize; bx++) {
            for (int by = 0; by < params.blockSize; by++) {
                T value {0};
                int x = params.x + bx;
                int y = params.y + by;
                if (x >= params.A->getSizeX() || y >= params.A->getSizeY()) { continue;}
                for (int i = 0; i < params.A->getSizeX(); i++) {
                    value += params.A->element(i, y) * params.B->element(x, i);
                }
                params.C->setElement(x,y,value);
            }
        }
    }
};
#endif // THREADEDMATRIXMULTIPLIER_H