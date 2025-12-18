#include <gtest/gtest.h>
#include <pcosynchro/pcotest.h>

#include "multipliertester.h"
#include "multiplierthreadedtester.h"
#include "threadedmatrixmultiplier.h"

#define ThreadedMultiplierType ThreadedMatrixMultiplier<int>

// Decommenting the next line allows to check for interlocking
// #define CHECK_DURATION

TEST(Multiplier, SingleThread){

#ifdef CHECK_DURATION
        ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                               constexpr int MATRIXSIZE = 500;
                               constexpr int NBTHREADS = 1;
                               constexpr int NBBLOCKSPERROW = 5;

                               MultiplierTester<ThreadedMultiplierType> tester;

                               tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                           }))
#endif // CHECK_DURATION

}


TEST(Multiplier, Simple){

#ifdef CHECK_DURATION
        ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                               constexpr int MATRIXSIZE = 500;
                               constexpr int NBTHREADS = 4;
                               constexpr int NBBLOCKSPERROW = 5;

                               MultiplierTester<ThreadedMultiplierType> tester;

                               tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                           }))
#endif // CHECK_DURATION

}


TEST(Multiplier, Reentering)
{

#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 500;
                           constexpr int NBTHREADS = 4;
                           constexpr int NBBLOCKSPERROW = 5;

                           MultiplierThreadedTester<ThreadedMultiplierType> tester(2);

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}
TEST(Multiplier, ReenteringWith3)
{

#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 500;
                           constexpr int NBTHREADS = 4;
                           constexpr int NBBLOCKSPERROW = 5;

                           MultiplierThreadedTester<ThreadedMultiplierType> tester(3);

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}
///
///\brief checks if function behaves correctly when matrixsize/nbblocks isn't a round number
///
TEST(Multiplier, OddNumber)
{

#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 501;
                           constexpr int NBTHREADS = 4;
                           constexpr int NBBLOCKSPERROW =7;

                           MultiplierThreadedTester<ThreadedMultiplierType> tester(1);

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}
///
///\brief tests if we can stop a multiplication halfway by using another thread
///
TEST(Multiplier, StopMidMultiplication)
{
    ASSERT_DURATION_LE(1, ({ // this test shouldn't be longer than 50ms, if it passes 1second it either hanged or is calculating the whole matrix
        constexpr int MATRIXSIZE = 800;
        constexpr int NBTHREADS = 4;
        constexpr int NBBLOCKSPERROW = 100;

        auto multiplier = std::make_unique<ThreadedMultiplierType>(NBTHREADS, NBBLOCKSPERROW);

        SquareMatrix<int> A(MATRIXSIZE);
        SquareMatrix<int> B(MATRIXSIZE);
        SquareMatrix<int> C(MATRIXSIZE);

        PcoThread computeThread([&]() {
            multiplier->multiply(A, B, C, NBBLOCKSPERROW);
        });

        PcoThread::usleep(10000);
        multiplier.reset(); //calls destroyer
        computeThread.join();

        ASSERT_TRUE(true);
    }))
}
///
///\brief making sure that extra threads will just sleep instantly
///
TEST(Multiplier, UnusedThreads)
{
#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
    constexpr int MATRIXSIZE = 200;
    constexpr int NBTHREADS = 8;
    constexpr int NBBLOCKSPERROW = 2;

    MultiplierTester<ThreadedMultiplierType> tester;
    tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);
#ifdef CHECK_DURATION
               }))
#endif // CHECK_DURATION
}




int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
