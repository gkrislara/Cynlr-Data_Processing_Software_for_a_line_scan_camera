#pragma once
#include "..\Shared\Platform.h"
#include "..\Shared\Data.h"
#include <iostream>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

struct Point {
	uint32_t x;
	uint32_t y;
};

struct FinalLabel {
	int32_t  label;       // component ID
	uint64_t size;        // total pixels
	Point    topLeft;     // (xMin, yMin)
	Point    bottomRight; // (xMax, yMax)
};

template<typename T, size_t dataWidth>
class Annotator : public CircularBufferFilterW<T> {

	struct LabelStats {
		T				   label = 0;
		uint64_t           labelSize = 0;
		Point              topMost = { UINT32_MAX, UINT32_MAX };
		Point              bottomMost = { 0,           0 };
		Point              leftMost = { UINT32_MAX, UINT32_MAX };
		Point              rightMost = { 0,           0 };
		uint64_t           lastSeen = 0;
		bool               active = false;
		std::vector<uint32_t> aliases;    // labels merged into this one
	};

	uint32_t                width;
	uint64_t                thresholdIters_;
	std::vector<LabelStats> labelStats_;        // per-label stats
	std::vector<T>  recycledLabels; // queued expirations
	std::vector<FinalLabel> finalisedLabels; // finalized labels

	/* Threading variables */
	std::thread labelEndThread;
	std::mutex labelEndMtx;
	std::condition_variable labelEndCv;
	std::atomic<bool> labelEndStop{ false };
	std::atomic<bool> labelEndWork{ false };
	std::mutex finalisedLabelsMtx;
	std::mutex recycledLabelsMtx;

	void ensureStatsSize(int32_t lbl) {
		if (lbl >= int(labelStats_.size()))
			labelStats_.resize(lbl + 1);
	}


	bool detectEnd(T curr) 
	{
		bool endDetected = false;
		auto& D = labelStats_[curr];

		std::cout << "buffer level: " << CircularBufferFilterW<T>::level() << ", last seen: " << D.lastSeen << ", threshold: " << thresholdIters_ << "\n";

		if (D.active) 
		{

			if(CircularBufferFilterW<T>::level() - D.lastSeen >= thresholdIters_)
			{
				endDetected = true;
			}
		}

		return endDetected;
	}

	/*TODO: verify */
	void mergeLabelStats(T oldLabel, T newLabel)
	{
		labelStats_[newLabel].labelSize += labelStats_[oldLabel].labelSize;
		labelStats_[newLabel].aliases.push_back(oldLabel);
		labelStats_[newLabel].aliases.insert(labelStats_[newLabel].aliases.end(),
			labelStats_[oldLabel].aliases.begin(),
			labelStats_[oldLabel].aliases.end());
		labelStats_[newLabel].topMost = {
			std::min(labelStats_[newLabel].topMost.x, labelStats_[oldLabel].topMost.x),
			std::min(labelStats_[newLabel].topMost.y, labelStats_[oldLabel].topMost.y)
		};
		labelStats_[newLabel].bottomMost = {
			std::max(labelStats_[newLabel].bottomMost.x, labelStats_[oldLabel].bottomMost.x),
			std::max(labelStats_[newLabel].bottomMost.y, labelStats_[oldLabel].bottomMost.y)
		};
		labelStats_[newLabel].leftMost = {
			std::min(labelStats_[newLabel].leftMost.x, labelStats_[oldLabel].leftMost.x),
			std::min(labelStats_[newLabel].leftMost.y, labelStats_[oldLabel].leftMost.y)
		};
		labelStats_[newLabel].rightMost = {
			std::max(labelStats_[newLabel].rightMost.x, labelStats_[oldLabel].rightMost.x),
			std::max(labelStats_[newLabel].rightMost.y, labelStats_[oldLabel].rightMost.y)
		};
		labelStats_[newLabel].lastSeen = std::max(labelStats_[newLabel].lastSeen, labelStats_[oldLabel].lastSeen);
		// reset old label stats
		labelStats_[oldLabel].aliases.clear();
		labelStats_[oldLabel].labelSize = 0;
		labelStats_[oldLabel].active = false;
		labelStats_[oldLabel].lastSeen = 0;
		labelStats_[oldLabel].topMost = { UINT32_MAX, UINT32_MAX };
		labelStats_[oldLabel].bottomMost = { 0,           0 };
		labelStats_[oldLabel].leftMost = { UINT32_MAX, UINT32_MAX };
		labelStats_[oldLabel].rightMost = { 0,           0 };

	}


	void detectEquivalenceAndMerge() 
	{
		uint16_t pos = (CircularBufferFilterW<T>::getHead() + CircularBufferFilterW<T>::capacity() - 1) % CircularBufferFilterW<T>::capacity();
		T curr = CircularBufferFilterW<T>::getBufferAt(pos);
		auto& D = labelStats_[curr];
		int sz = int(CircularBufferFilterW<uint16_t>::capacity());

		int nbr[4] = {
			(pos + sz - 1) % sz,  // left
			(pos + sz - int(width) - 1) % sz,  // top-left
			(pos + sz - int(width)) % sz,  // top
			(pos + sz - int(width) + 1) % sz   // top-right
		};

		for (int i = 0; i < 4; ++i) {
			T nlab = CircularBufferFilterW<T>::getBufferAt(nbr[i]);
			if (nlab > 0 && nlab != curr && nlab < int(labelStats_.size())) {
				auto& S = labelStats_[nlab];
				if (!S.active) continue;
				mergeLabelStats(nlab, curr);
			}
		}
	}

	void resetStats(T label) 
	{
		if (label == 0) return;
		labelStats_[label].labelSize = 0;
		labelStats_[label].topMost = { UINT32_MAX, UINT32_MAX };
		labelStats_[label].bottomMost = { 0,           0 };
		labelStats_[label].leftMost = { UINT32_MAX, UINT32_MAX };
		labelStats_[label].rightMost = { 0,           0 };
		labelStats_[label].lastSeen = 0;
		labelStats_[label].active = false;
		labelStats_[label].aliases.clear();
	}

	void initializeStat(T label, uint32_t row, uint32_t col)
	{
		if (label == 0) return;
		labelStats_[label].label = label;
		labelStats_[label].labelSize = 0;
		labelStats_[label].topMost = { col,row };
		labelStats_[label].bottomMost = { col,row };
		labelStats_[label].leftMost = { col, row };
		labelStats_[label].rightMost = { col, row };
		labelStats_[label].lastSeen = 0;
		labelStats_[label].active = true;
		labelStats_[label].aliases.clear();
	}
	

	void computeSize(T label) 
	{
		if (label == 0) return;

		labelStats_[label].labelSize++;

	}

	void computeCoordinates(T label,uint32_t row, uint32_t col)
	{
		if (row < labelStats_[label].topMost.y
			|| (row == labelStats_[label].topMost.y && col < labelStats_[label].topMost.x))
			labelStats_[label].topMost = { col, row };
		if (row > labelStats_[label].bottomMost.y)
			labelStats_[label].bottomMost = { col, row };
		if (col < labelStats_[label].leftMost.x
			|| (col == labelStats_[label].leftMost.x && row < labelStats_[label].leftMost.y))
			labelStats_[label].leftMost = { col, row };
		if (col > labelStats_[label].rightMost.x)
			labelStats_[label].rightMost = { col, row };
	}

	/* thread candidate */
	void detectAndHandleLabelEnding()
	{
		std::cout << "+";
		for (T lbl = 1; lbl < labelStats_.size(); ++lbl)
		{

			if (detectEnd(lbl))
			{
				// append to output list
				{
					std::lock_guard<std::mutex> lock(finalisedLabelsMtx);
					std::cout << "Finalising label " << int(lbl) << " of size " << labelStats_[lbl].labelSize << "\n";
					finalisedLabels.push_back(
						FinalLabel{
							labelStats_[lbl].label,
							labelStats_[lbl].labelSize,
							{
								labelStats_[lbl].leftMost.x,
								labelStats_[lbl].topMost.y
							},
							{
								labelStats_[lbl].rightMost.x,
								labelStats_[lbl].bottomMost.y
							}
						});
				}
				// mark for recycling
				{
					std::lock_guard<std::mutex> lock(recycledLabelsMtx);
					std::cout << "Recycling label " << int(lbl) << "\n";
					recycledLabels.push_back(lbl);
					//pushback its aliases too
					for (T al : labelStats_[lbl].aliases)
					{
						recycledLabels.push_back(al);
					}
					/*Notify interface*/
				}
				// reset stats
				resetStats(lbl);
			}
		}
	}

	void labelEndThreadFunc() {
		while (true/*!labelEndStop*/) {
			std::unique_lock<std::mutex> lock(labelEndMtx);
			labelEndCv.wait(lock, [&] { return labelEndWork || labelEndStop; });
			if (labelEndStop) break;
			detectAndHandleLabelEnding();
			labelEndWork = false;
		}
	}


public:
	Annotator(uint32_t m, uint64_t thresholdIters) :
		CircularBufferFilterW<T>(m + dataWidth + 1, 0),
		width(m)
	{
		thresholdIters_ = width + 1;

		/* initialise thread*/
		labelEndThread = std::thread(&Annotator::labelEndThreadFunc, this);
	}

	~Annotator() = default;

	void algorithm(T label, uint32_t row, uint32_t col) 
	{

		/* get head -1*/
		uint16_t pos = (CircularBufferFilterW<T>::getHead() + CircularBufferFilterW<T>::capacity() - 1) % CircularBufferFilterW<T>::capacity();

		T bufLabel = CircularBufferFilterW<T>::getBufferAt(pos);

		if (bufLabel != label) 
		{
			return; // not the right position to update stats
		}

		ensureStatsSize(label);


		if (!labelStats_[label].active) {
			initializeStat(label, row, col);
		}

		// update stat
		computeSize(label);
		labelStats_[label].lastSeen = CircularBufferFilterW<T>::level();
		
		computeCoordinates(label, row, col);
	}

	/* thread candidate */
	bool popFinalisedLabel(FinalLabel& outLabel) 
	{
		std::lock_guard<std::mutex> lock(finalisedLabelsMtx);
		if (finalisedLabels.empty()) {
			outLabel = FinalLabel{ 0,0,{0,0},{0,0} };
			return false;
		}
		outLabel = finalisedLabels.back();
		finalisedLabels.pop_back();
		return true;
	}

	/* thread candidate */
	bool popRecycledLabel(T& outLabel) 
	{
		std::lock_guard<std::mutex> lock(recycledLabelsMtx);
		if (recycledLabels.empty()) {
			outLabel = 0;
			return false;
		}
		outLabel = recycledLabels.back();
		recycledLabels.pop_back();
		return true;
	}


	void requestLabelEndingDetection() {
		labelEndWork = true;
		labelEndCv.notify_one();
	}

	/* label stat size*/
	int getStatSize() const {
		return int(labelStats_.size());
	}


};