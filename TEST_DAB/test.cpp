#include "pch.h"
#include "DAB.h"

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



static void runAnnotatork2(
    uint32_t width,
    uint32_t height,
    const std::vector<uint8_t>& image, const std::vector<uint16_t>& labels)
{
    uint8_t k = 2;
    Annotator<uint16_t,2> annotation(width,width+1);
    std::vector<uint16_t> recycleLabels;
	std::vector<FinalLabel> computedlabels;


    size_t avg_mem = 0;
    size_t avg_latency = 0;
    int idx = 0;
    while (idx < int(image.size())) {
        Pixel inBlock[2], outBlock[2];
        for (int i = 0; i < k; ++i) {
            inBlock[i] = { image[idx], idx / width, uint32_t(idx % width), labels[idx]};
            annotation.push(inBlock[i].label.labelValue);
			annotation.requestLabelEndingDetection(); // to detect end of labels
            ++idx;

            auto mem_before = getCurrentMemoryUsageKB();
            auto start = std::chrono::high_resolution_clock::now();

            annotation.algorithm(static_cast<uint16_t>(inBlock[i].label), inBlock[i].row, inBlock[i].col);

            auto end = std::chrono::high_resolution_clock::now();
            auto mem_after = getCurrentMemoryUsageKB();
            auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            EXPECT_LE(duration_ns, 500);
            avg_latency += duration_ns;
            avg_mem += (mem_after - mem_before); 
        }
    }

	std::cout << "Label stat size: " << annotation.getStatSize() << std::endl;

    uint16_t rcyclLbl;
    std::cout << "Recycled Label: ";
    while(annotation.popRecycledLabel(rcyclLbl))
    {
		recycleLabels.push_back(rcyclLbl);
		std::cout << rcyclLbl << " ";
    }
	std::cout << std::endl;

	FinalLabel fLbl;
	std::cout << "Finalised Label: ";
    while (annotation.popFinalisedLabel(fLbl))
    {
		computedlabels.push_back(fLbl);
		std::cout << "(" << fLbl.label << "," << fLbl.size << ") ";
    }
	std::cout << std::endl;

    avg_latency /= int(image.size());
    avg_mem /= int(image.size());
    std::cout << "Average Duration: " << avg_latency << " ns" << std::endl;
    std::cout << "Average Memory: " << avg_mem << " KB" << std::endl;
    return ;
}


TEST(CCL_SingleRow, HorizontalRuns) {
    std::vector<uint8_t> img = { 1,0,1,0,1,0,0,0,0,0,0,0 };
    std::vector<uint16_t> expect = { 1,0,2,0,3,0,0,0,0,0,0,0 };
    runAnnotatork2(6, 1, img,expect);
}

//TEST(CCL_SingleColumn, VerticalRuns) {
//    std::vector<uint8_t> img = { 1,1,0,1 };
//    auto labels = runCCLk2(4, 1, img);
//    std::vector<uint32_t> expect = { 1,1,0,2 };
//    EXPECT_EQ(labels, expect);
//}
//
//TEST(CCL_Block2x2_AllOnes) {
//    std::vector<uint8_t> img = {
//        1,1,
//        1,1
//    };
//    auto labels = runCCLk2(2, 2, img);
//    std::vector<uint32_t> expect = { 1,1, 1,1 };
//    EXPECT_EQ(labels, expect);
//}
//
//TEST(CCL_Block2x2_Diagonal) {
//    std::vector<uint8_t> img = {
//        1,0,
//        0,1
//    };
//    auto labels = runCCLk2(2, 2, img);
//    std::vector<uint32_t> expect = { 1,0, 0,1 };
//    EXPECT_EQ(labels, expect);
//}
//
//TEST(CCL_LShape, LShapeMerge) {
//    std::vector<uint8_t> img = {
//        1,0,0,1,
//        1,1,0,0
//    };
//    auto labels = runCCLk2(4, 2, img);
//    std::vector<uint32_t> expect = { 1,0,0,2, 1,1,0,0 };
//    EXPECT_EQ(labels, expect);
//}
//
//TEST(CCL_Checkerboard3x3_And_A_Seperate_Region_With_Perf, AllConnectedByDiagonal) {
//    std::vector<uint8_t> img = {
//      1,0,1,0,1,0,0,1,1,1,0,1,1,0,
//      0,1,0,1,0,1,0,1,1,1,0,1,1,0,
//      1,0,1,0,1,0,0,1,1,1,0,1,1,0
//    };
//
//    auto labels = runCCLk2(14, 3, img);
//
//    // all 1s merge via diagonal => all get label 1
//    std::vector<uint32_t> expect = {
//      1,0,2,0,3,0,0,4,4,4,0,5,5,0,
//      0,1,0,1,0,1,0,4,4,4,0,5,5,0,
//      1,0,1,0,1,0,0,4,4,4,0,5,5,0
//    };
//    EXPECT_EQ(labels, expect);
//
//}
//
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
//
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