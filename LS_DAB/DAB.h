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
		bool               active = false;
		bool			   redundant_ = false;
		std::vector<uint32_t> aliases;    // labels merged into this one
	};

	uint32_t                width;
	uint64_t                thresholdIters_;
	std::vector<LabelStats> labelStats_;        // per-label stats
	std::vector<T>  recycledLabels; // queued expirations
	std::vector<FinalLabel> finalisedLabels; // finalized labels
	std::mutex recycledLabelsMtx;
	std::mutex finalisedLabelsMtx;


	void ensureStatsSize(T lbl) {
		if (lbl >= int(labelStats_.size()))
			labelStats_.resize(lbl + 1);
	}


	T detectEnd() 
	{
		bool endDetected = false;

		size_t tail = CircularBufferFilterW<T>::getTail();

		if (!CircularBufferFilterW<T>::isFull()) return endDetected;

		size_t currTail = (tail - 1) % CircularBufferFilterW<T>::capacity();

		T currLbl = CircularBufferFilterW<T>::getBufferAt(currTail);
		auto& D = labelStats_[currLbl];

		if (D.active) 
		{
			std::wcout << L"Label end detection curr label: " << currLbl << std::endl;
			if (CircularBufferFilterW<T>::bufCount(currLbl) == 1) endDetected = true;
		}

		if (endDetected) return currLbl;
		else return static_cast<T>(0);
	}

	/*TODO: verify */
	void mergeLabelStats(T oldLabel, T newLabel)
	{
		labelStats_[oldLabel].labelSize += labelStats_[newLabel].labelSize;
		labelStats_[oldLabel].aliases.push_back(newLabel);

		//
		labelStats_[oldLabel].aliases.insert(labelStats_[oldLabel].aliases.end(),
			labelStats_[newLabel].aliases.begin(),
			labelStats_[newLabel].aliases.end());
		//

		labelStats_[oldLabel].topMost = {
			std::min(labelStats_[newLabel].topMost.x, labelStats_[oldLabel].topMost.x),
			std::min(labelStats_[newLabel].topMost.y, labelStats_[oldLabel].topMost.y)
		};
		labelStats_[oldLabel].bottomMost = {
			std::max(labelStats_[newLabel].bottomMost.x, labelStats_[oldLabel].bottomMost.x),
			std::max(labelStats_[newLabel].bottomMost.y, labelStats_[oldLabel].bottomMost.y)
		};
		labelStats_[oldLabel].leftMost = {
			std::min(labelStats_[newLabel].leftMost.x, labelStats_[oldLabel].leftMost.x),
			std::min(labelStats_[newLabel].leftMost.y, labelStats_[oldLabel].leftMost.y)
		};
		labelStats_[oldLabel].rightMost = {
			std::max(labelStats_[newLabel].rightMost.x, labelStats_[oldLabel].rightMost.x),
			std::max(labelStats_[newLabel].rightMost.y, labelStats_[oldLabel].rightMost.y)
		};
		// reset new label stats
		resetStats(newLabel);

	}


	T detectEquivalence(T lbl,uint16_t pos) 
	{
		// change logic
		T olab;
		auto& D = labelStats_[lbl];
		int sz = int(CircularBufferFilterW<uint16_t>::capacity());

		int nbr[4] = {
			(pos + sz - 1) % sz,  // left
			(pos + sz - int(width) - 1) % sz,  // top-left
			(pos + sz - int(width)) % sz,  // top
			(pos + sz - int(width) + 1) % sz   // top-right
		};

		for (int i = 0; i < 4; ++i) {
			olab = CircularBufferFilterW<T>::getBufferAt(nbr[i]);
			if (olab > 0 && olab != lbl && olab < int(labelStats_.size())) {
				auto& S = labelStats_[olab];
				if (!S.active) continue;
				D.redundant_ = true;
			}
		}

		if (D.redundant_) return  olab;
		else return lbl;
	}

	void resetStats(T label) 
	{
		if (label == 0) return;
		labelStats_[label].labelSize = 0;
		labelStats_[label].topMost = { UINT32_MAX, UINT32_MAX };
		labelStats_[label].bottomMost = { 0,           0 };
		labelStats_[label].leftMost = { UINT32_MAX, UINT32_MAX };
		labelStats_[label].rightMost = { 0,           0 };
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


	void labelEndProc(T lbl)
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
		}
		resetStats(lbl);
	}

public:
	Annotator(uint32_t m) :
		CircularBufferFilterW<T>(m + dataWidth + 1, 0),
		width(m)
	{
		thresholdIters_ = width + 1;

		recycledLabels.reserve(1024); // initial capacity, can grow as needed
		finalisedLabels.reserve(1024); // initial capacity, can grow as needed
		labelStats_.reserve(2048);
		
	}

	~Annotator() = default;

	void algorithm(T label, uint32_t row, uint32_t col) 
	{
		if (label == 0) return;
		size_t bufHead = CircularBufferFilterW<T>::getHead();
		size_t currHeadm1 = (bufHead + CircularBufferFilterW<T>::capacity() - 1) % CircularBufferFilterW<T>::capacity();
		size_t bufTail = CircularBufferFilterW<T>::getTail();

		T bufLabel = CircularBufferFilterW<T>::getBufferAt(currHeadm1);
		
	
		if(bufLabel != label)
		{
			return;
		}

		ensureStatsSize(label);

		if (!labelStats_[label].active) {
			initializeStat(label, row, col);
		}

		// update stat
		computeSize(label);

		if (!CircularBufferFilterW<T>::isFull())
		{
			computeCoordinates(label, row, col);
			return; // initial m+dw+1 turns will exit at this point and buffer will run at full capcity after this
		}

		// 3. detectmerge of label at head update labelstat with merged label
		
		T eqLbl = detectEquivalence(label, currHeadm1); // not needed as labels are merged inplace in the labeller .. anyways for sanity and future proofing

		// 4. compute stats of label at head and merge if detect merge was true
		
		computeCoordinates(label, row, col);
		if (labelStats_[label].redundant_)
			mergeLabelStats(eqLbl, label);

		// 5. detect end: get buf count (buf.tail) if 1 that label has ended; placed here for synchronous ending detection
		T endLbl = detectEnd();
		// 7. push label to finalised and recycled
		// 8. reset stats
		if (endLbl)
		{
			labelEndProc(endLbl);
		}
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

	/* label stat size*/
	int getStatSize() const {
		return int(labelStats_.size());
	}


};