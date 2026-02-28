#include "pch.h"
#include <chrono>
#include "DFB.h"


TEST(DFB_FILTER_PERF_MOCK, DFB) {
	//TEST_OK

	constexpr std::array<float, 3> coeffs = { 0.2f, 0.3f, 0.5f };
	FilterCoefficients<float>* mockCoeffs = new DynamicFilterCoefficients<float>(coeffs);

	PreProcessor<float> preProcessor(mockCoeffs);
	
	float arr[3] = { 1.0f, 2.0f , 3.0f };

	auto start = std::chrono::high_resolution_clock::now();
	
	preProcessor.push(arr[0]);
	preProcessor.push(arr[1]);
	preProcessor.push(arr[2]);

	float data = preProcessor.algorithm();

	auto end = std::chrono::high_resolution_clock::now();

	auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	std::cout << preProcessor.getBufferAt(0) << " " << preProcessor.getBufferAt(1) << " " << preProcessor.getBufferAt(2) << std::endl;

	std::cout << "Data:" << (float)data << " Duration: " << duration_ns << " ns" << std::endl;
	EXPECT_LE(duration_ns, 500) << "with " << duration_ns - 500 << " difference";

}

TEST(DFB_FILTER_PERF_PS1_COEFF, DFB) {
	//TEST_OK

	FilterCoefficients<float>* coeffs = new DynamicFilterCoefficients<float>(PS1_COEFFS_FLOAT);

	PreProcessor<float> preProcessor(coeffs);

	float arr[9] = { 1.0f, 2.0f , 3.0f , 4.0f, 1.0f, 2.0f , 3.0f , 4.0f, 5.0f };

	auto start = std::chrono::high_resolution_clock::now();

	preProcessor.push(arr[0]);
	preProcessor.push(arr[1]);
	preProcessor.push(arr[2]);
	preProcessor.push(arr[3]);
	preProcessor.push(arr[4]);
	preProcessor.push(arr[5]);
	preProcessor.push(arr[6]);
	preProcessor.push(arr[7]);
	preProcessor.push(arr[8]);

	float data = preProcessor.algorithm();

	auto end = std::chrono::high_resolution_clock::now();

	auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	std::cout << preProcessor.getBufferAt(3) << " " << preProcessor.getBufferAt(7) << " " << preProcessor.getBufferAt(8) << std::endl;

	std::cout << "Data:" << (float)data << " Duration: " << duration_ns << " ns" << std::endl;
	EXPECT_LE(duration_ns, 500) << "with " << duration_ns - 500 << " difference";

}

TEST(DFB_FILTER_PERF_PS1_COEFF_STREAM_FILTER, DFB)
{

}
