#include "pch.h"
#include "DLB_v0_1.h"

#include "..\Shared\data.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#include <chrono>

size_t getCurrentMemoryUsageKB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / 1024;
    }
#endif
    return 0; // Not implemented for non-Windows
}

// -----------------------------------------------------------------------------------------------------------------------
// This a test for the Connected Component Labelling (CCL) algorithm; CCL only test not with Tracing and computation block
// CCL with Tracing and computation block uses recycled labels
// Helper to run connected component labelling on a full image (k=2) and collect labels
// ------------------------------------------------------------------------------------------------------------------------
static std::vector<uint32_t> runCCLk2(
    uint32_t width,
    uint32_t height,
    const std::vector<uint8_t>& image)
{
    uint8_t k = 2;
    ConnectedComponentLabeller<2> ccl(width);
    std::vector<uint32_t> labels;
    labels.reserve(width * height);

	size_t avg_mem = 0;
	size_t avg_latency = 0;
    int idx = 0;

    while (idx < int(image.size())) {
        Pixel inBlock[2], outBlock[2];
        for (int i = 0; i < k; ++i) {
            inBlock[i] = { image[idx], idx / width, uint32_t(idx % width), 0};

            auto mem_before = getCurrentMemoryUsageKB();
            auto start = std::chrono::high_resolution_clock::now();

            inBlock[i].label = ccl.algorithm(static_cast<uint16_t>(inBlock[i].value));
		

            auto end = std::chrono::high_resolution_clock::now();
            auto mem_after = getCurrentMemoryUsageKB();
            auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            //EXPECT_LE(duration_ns, 500);
			avg_latency += duration_ns;
			avg_mem += (mem_after - mem_before);
            
            labels.push_back(inBlock[i].label);
            ++idx;

        }
    }
	avg_latency /= int(image.size());
    avg_mem /= int(image.size());
	std::cout << "Average Duration: " << avg_latency << " ns" << std::endl;
	std::cout << "Average Memory: " << avg_mem << " KB" << std::endl;
    return labels;
}


// ----------------------------------------------------------------
//                         Test Cases for CCL
// ----------------------------------------------------------------

TEST(CCL_SingleRow, HorizontalRuns) {
    std::vector<uint8_t> img = { 1,0,1,0,1,0 };
    auto labels = runCCLk2(6, 1, img);
    std::vector<uint32_t> expect = { 1,0,2,0,3,0 };
    EXPECT_EQ(labels, expect);
}

TEST(CCL_SingleColumn, VerticalRuns) {
    std::vector<uint8_t> img = { 1,1,0,1};
    auto labels = runCCLk2(1, 4, img);
    std::vector<uint32_t> expect = { 1,1,0,2};
    EXPECT_EQ(labels, expect);
}

TEST(CCL_Block2x2_AllOnes) {
    std::vector<uint8_t> img = {
        1,1,
        1,1
    };
    auto labels = runCCLk2(2, 2, img);
    std::vector<uint32_t> expect = { 1,1, 1,1 };
    EXPECT_EQ(labels, expect);
}

TEST(CCL_Block2x2_Diagonal) {
    std::vector<uint8_t> img = {
        1,0,
        0,1
    };
    auto labels = runCCLk2(2, 2, img);
    std::vector<uint32_t> expect = { 1,0, 0,1 };
    EXPECT_EQ(labels, expect);
}

TEST(CCL_LShape, LShapeMerge) {
    std::vector<uint8_t> img = {
        1,0,0,1,
        1,1,0,0
    };
    auto labels = runCCLk2(4, 2, img);
    std::vector<uint32_t> expect = { 1,0,0,2, 1,1,0,0 };
    EXPECT_EQ(labels, expect);
}

TEST(CCL_Checkerboard3x3_And_A_Seperate_Region_With_Perf, AllConnectedByDiagonal) {
    std::vector<uint8_t> img = {
      1,0,1,0,1,0,0,1,1,1,0,1,1,0,
      0,1,0,1,0,1,0,1,1,1,0,1,1,0,
      1,0,1,0,1,0,0,1,1,1,0,1,1,0
    };

    auto labels = runCCLk2(14, 3, img);

    // all 1s merge via diagonal => all get label 1
    std::vector<uint32_t> expect = {
      1,0,2,0,3,0,0,4,4,4,0,5,5,0,
      0,1,0,1,0,1,0,4,4,4,0,5,5,0,
      1,0,1,0,1,0,0,4,4,4,0,5,5,0
    };
    EXPECT_EQ(labels, expect);

}

//TEST(CCL_PS_With_Perf) {
//    // Same as problem statement 
//    std::vector<uint8_t> img = {
//       0,1,0,0,0,0,1,1,0,1,0,1,
//       0,0,1,0,0,0,1,0,0,1,1,0,
//       1,0,0,1,0,1,0,0,0,1,0,0,
//       0,1,1,0,0,1,0,0,1,0,0,1,
//       0,1,0,1,0,0,1,1,0,0,1,0,
//       0,0,0,0,0,0,0,0,1,0,0,0
//    };
//
//
//    auto labels = runCCLk2(12, 6, img);
//
//
//
//    // 5 is not replaced by 1 as the connected is scanned clockwise from near left to top right. Merging happens in the Tracing and computation block
//    std::vector<uint32_t> expect = {
//        0,1,0,0,0,0,2,2,0,3,0,4,
//        0,0,1,0,0,0,2,0,0,3,3,0,
//        5,0,0,1,0,2,0,0,0,3,0,0,
//        0,5,5,0,0,2,0,0,3,0,0,6,
//        0,5,0,5,0,0,2,2,0,0,6,0,
//        0,0,0,0,0,0,0,0,2,0,0,0
//    };
//    /*  EXPECT_EQ(labels, expect);*/
//    //for (size_t i = 0; i < 12 * 6; ++i) {
//    EXPECT_EQ(labels, expect);
//    //}
//}

//TEST(CCL_ZigZag2x4, SingleChain) {
//    // Zig-zag chain in a 2x4 image:
//    // 1 0
//    // 0 1
//    // 1 0
//    // 0 1
//    std::vector<uint8_t> img = {
//      1,0,
//      0,1,
//      1,0,
//      0,1
//    };
//    auto labels = runCCLk2(2, 4, img);
//    // each 1 connects diagonally to the previous => all 1s label=1
//    std::vector<uint32_t> expect = {
//      1,0,
//      0,1,
//      1,0,
//      0,1
//    };
//    EXPECT_EQ(labels, expect);
//}


/* Add test cases from the block */

//TEST(INTEGRATION_TEST, INTG)
//{ 
//    std::vector<uint8_t> img = {
//        0,0,0,0,0,0,0,0,0,1,1,
//        1,1,0,0,0,0,0,1,1,1,1,
//        1,1,1,1,1,0,1,1,1,1,1,
//        0,0,0,0,0,0,0,0,0,0,0,
//        0,1,1,1,0,0,0,0,1,1,1,
//        1,1,1,1,1,1,0,1,1,0,0,
//        0,0,0,1,1,1,0,0,1,0,0,
//        1,1,1,1,1,1,0,0,0,0,0,
//        1,0,0,0,0,0,1,1,1,1,1,
//        1,1,1,1,1,1,0,0,0,0,1,
//        1,1,1,1,1,0,0,0,0,1,1,
//        0,0,0,0,0,0,1,1,0,1,1,
//        0,0,1,1,1,1,1,1,1,1,0,
//        0,0,0,0,0,0,1,1,1,1,1,
//        1,1,1,1,1,1,1,1,1,1,0,
//        0,0,0,0,0,0,0,0,0,0,0,
//        0,0,0,1,1,1,1,1,1,1,0,
//        0,0,0,0,0,1,1,1,1,1,1,
//        1,1,1,1,1,1,1,1,1,1,1,
//        0,0,0,0,0,0,0,0,1,1,1,
//        1,1,1,1,1,0,0,0,1,1,1,
//        1,1,0,0,0,0,0,1,1,1,1,
//        0,0,0,0,0,0,0,0,0,0,1,
//        1,1,0,0,1,1,1,1,1,1,0,
//        0,0,0,1,1,1,1,0,0,0,0,
//        0,1,1,1,1,0,0,0,0,0,0,
//        0,0,0,1,0,0,0,1,1,1,1,
//        1,1,0,0,0,0,0,0,0,1,1,
//        1,1,0,0,0,1,1,1,1,0,0,
//        0,1,1,1,0,0,1,0,0,0,1,
//        1,0,0,1,1,1,0,0,0,1,0,
//        0,0,0,1,1,1,1,1,0,0,1,
//        1,1,1,1,1,1,0,0,0,1,1
//    };
//
//    auto labels = runCCLk2(11, 33, img);
//
//    std::vector<uint32_t> expect = {
//        0,0,0,0,0,0,0,0,0,1,1,
//        2,2,0,0,0,0,0,1,1,1,1,
//        2,2,2,2,2,0,1,1,1,1,1,
//        0,0,0,0,0,0,0,0,0,0,0,
//        0,3,3,3,0,0,0,0,4,4,4,
//        3,3,3,3,3,3,0,4,4,0,0,
//        0,0,0,3,3,3,0,0,4,0,0,
//        5,5,5,3,3,3,0,0,0,0,0,
//        5,0,0,0,0,0,3,3,3,3,3,
//        3,3,3,3,3,3,0,0,0,0,3,
//        3,3,3,3,3,0,0,0,0,3,3,
//        0,0,0,0,0,0,6,6,0,3,3,
//        0,0,7,7,7,3,3,3,3,3,0,
//        0,0,0,0,0,0,3,3,3,3,3,
//        8,8,8,8,8,8,3,3,3,3,0,
//        0,0,0,0,0,0,0,0,0,0,0,
//        0,0,0,9,9,9,9,9,9,9,0,
//        0,0,0,0,0,9,9,9,9,9,9,
//        10,10,10,10,9,9,9,9,9,9,9,
//        0,0,0,0,0,0,0,0,9,9,9,
//        11,11,11,11,11,0,0,0,9,9,9,
//        11,11,0,0,0,0,0,9,9,9,9,
//        0,0,0,0,0,0,0,0,0,0,9,
//        12,12,0,0,13,13,13,13,13,9,0,
//        0,0,0,9,9,9,9,0,0,0,0,
//        0,14,9,9,9,0,0,0,0,0,0,
//        0,0,0,9,0,0,0,15,15,15,15,
//        16,16,0,0,0,0,0,0,0,15,15,
//        16,16,0,0,0,17,17,17,15,0,0,
//        0,16,16,16,0,0,15,0,0,0,18,
//        16,0,0,16,16,15,0,0,0,18,0,
//        0,0,0,15,15,15,15,15,0,0,18,
//        19,19,19,15,15,15,0,0,0,18,18
//    };
//
//    EXPECT_EQ(labels, expect);
//}
